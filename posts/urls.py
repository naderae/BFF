from . import views
from django.urls import path


app_name = 'posts'

urlpatterns = [
    path('create/<int:group_id>/', views.create, name='create'),
    path('edit/<int:group_id>/<int:post_id>/', views.edit, name='edit'),
    path('delete/<int:group_id>/<int:post_id>/', views.delete, name='delete'),
    path('like/<int:group_id>/<int:post_id>/', views.like, name='like'),
]

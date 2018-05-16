from . import views
from django.urls import path, include
from django.conf.urls import url

app_name = 'groups'

urlpatterns = [
    path('create/', views.create, name='create'),
    path('index/', views.index, name='index'),

    # url(r'^detail/(?P<group_id>\d+)/$', views.detail, name='detail'),
    # url(r'^detail/(?P<group_id>\d+)/(?P<post_id>\d+)/$', views.detail, name='detail'),


    path('<int:group_id>/', views.detail, name='detail'),
    path('<int:group_id>/<int:post_id>/', views.detail, name='detail'),

    path('<int:group_id>/join/', views.join, name='join'),
    path('<int:group_id>/leave/', views.leave, name='leave'),

]
